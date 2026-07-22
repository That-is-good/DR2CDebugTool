using DR2CDebugTool.Models;
using DR2CDebugTool.Services;
using System;
using System.Collections.ObjectModel;
using System.Linq;

namespace DR2CDebugTool.ViewModels
{
    public class EntityViewModel : BaseViewModel
    {
        private readonly MemoryService _memory;
        private const int MaxEntityScanCount = 500;
        private const int EntityCountAddressOffset = 0x3CC318;

        public ObservableCollection<EntityInfo> AllEntities { get; } = [];
        public ObservableCollection<EntityInfo> FilteredEntities { get; } = [];

        private EntityInfo? _currentEntity;
        public EntityInfo? CurrentEntity
        {
            get => _currentEntity;
            set
            {
                _currentEntity = value;
                OnPropertyChanged();
                if (value != null) UpdateEntityUI(value);
            }
        }

        private EntityInfo? _targetEntity;
        public EntityInfo? TargetEntity
        {
            get => _targetEntity;
            set
            {
                _targetEntity = value;
                OnPropertyChanged();
                UpdateTargetStatus();
            }
        }

        private string _entityStatus = "";
        public string EntityStatus
        {
            get => _entityStatus;
            set => SetProperty(ref _entityStatus, value);
        }

        private string _entityCountText = LanguageManager.Combine("Entities", "0", ": ");
        public string EntityCountText
        {
            get => _entityCountText;
            set => SetProperty(ref _entityCountText, value);
        }

        private string _targetStatusText = "";
        public string TargetStatusText
        {
            get => _targetStatusText;
            set => SetProperty(ref _targetStatusText, value);
        }

        // Filter properties
        private int _typeFilterIndex;
        public int TypeFilterIndex
        {
            get => _typeFilterIndex;
            set { SetProperty(ref _typeFilterIndex, value); ApplyFilter(); }
        }

        private string _areaFilter = LanguageManager.Get("ENTITY_TYPE_ALL");
        public string AreaFilter
        {
            get => _areaFilter;
            set { SetProperty(ref _areaFilter, value); ApplyFilter(); }
        }

        private string _searchText = "";
        public string SearchText
        {
            get => _searchText;
            set { SetProperty(ref _searchText, value); ApplyFilter(); }
        }

        public ObservableCollection<string> AreaOptions { get; } = [LanguageManager.Get("ENTITY_TYPE_ALL")];

        // Entity detail properties
        private ushort _detailEntityId;
        public ushort DetailEntityId { get => _detailEntityId; set => SetProperty(ref _detailEntityId, value); }

        private string _detailTypeName = "";
        public string DetailTypeName { get => _detailTypeName; set => SetProperty(ref _detailTypeName, value); }

        private string _detailSubTypeName = "";
        public string DetailSubTypeName { get => _detailSubTypeName; set => SetProperty(ref _detailSubTypeName, value); }

        private int _detailHealth;
        public int DetailHealth { get => _detailHealth; set => SetProperty(ref _detailHealth, value); }

        private byte _detailAreaId;
        public byte DetailAreaId { get => _detailAreaId; set => SetProperty(ref _detailAreaId, value); }

        private float _detailPosX, _detailPosY, _detailPosZ;
        private float _detailVelX, _detailVelY, _detailVelZ;
        public float DetailPosX { get => _detailPosX; set => SetProperty(ref _detailPosX, value); }
        public float DetailPosY { get => _detailPosY; set => SetProperty(ref _detailPosY, value); }
        public float DetailPosZ { get => _detailPosZ; set => SetProperty(ref _detailPosZ, value); }
        public float DetailVelX { get => _detailVelX; set => SetProperty(ref _detailVelX, value); }
        public float DetailVelY { get => _detailVelY; set => SetProperty(ref _detailVelY, value); }
        public float DetailVelZ { get => _detailVelZ; set => SetProperty(ref _detailVelZ, value); }

        // Debug properties
        private bool _detailNoCollide;
        public bool DetailNoCollide { get => _detailNoCollide; set => SetProperty(ref _detailNoCollide, value); }
        private bool _detailInvisible;
        public bool DetailInvisible { get => _detailInvisible; set => SetProperty(ref _detailInvisible, value); }
        private bool _detailInvincible;
        public bool DetailInvincible { get => _detailInvincible; set => SetProperty(ref _detailInvincible, value); }
        private bool _detailGlow;
        public bool DetailGlow { get => _detailGlow; set => SetProperty(ref _detailGlow, value); }
        private float _detailMass;
        public float DetailMass { get => _detailMass; set { SetProperty(ref _detailMass, value); ApplyEntityDebug(); } }
        private float _detailFriction;
        public float DetailFriction { get => _detailFriction; set { SetProperty(ref _detailFriction, value); ApplyEntityDebug(); } }
        private int _detailAIState;
        public int DetailAIState { get => _detailAIState; set { SetProperty(ref _detailAIState, value); ApplyEntityDebug(); } }
        private int _detailAIWait;
        public int DetailAIWait { get => _detailAIWait; set { SetProperty(ref _detailAIWait, value); ApplyEntityDebug(); } }

        public EntityViewModel(MemoryService memory)
        {
            _memory = memory;
        }

        public void ScanEntities()
        {
            if (!_memory.IsReady) { EntityStatus = "Not attached"; return; }

            AllEntities.Clear();
            try
            {
                IntPtr countAddr = _memory.ModuleBase + EntityCountAddressOffset;
                int entityCount = _memory.ReadInt32(countAddr);

                if (entityCount <= 0 || entityCount > MaxEntityScanCount)
                {
                    EntityStatus = $"Invalid entity count: {entityCount}";
                    return;
                }

                IntPtr poolStart = _memory.ModuleBase + (int)_memory.Settings.EntityPoolOffset;
                uint foundCount = 0;

                for (int i = 0; i < _memory.Settings.EntitySlots && foundCount < entityCount; i++)
                {
                    IntPtr entityAddr = poolStart + i * (int)_memory.Settings.EntitySize;
                    ushort entityId = _memory.ReadUInt16(entityAddr);
                    if (entityId == 0) continue;

                    var entity = ReadEntityData(entityAddr);
                    if (entity == null) continue;

                    entity.Index = i;
                    AllEntities.Add(entity);
                    ++foundCount;
                }

                UpdateAreaFilter();
                ApplyFilter();
                EntityStatus = LanguageManager.Combine("Total_Entity", AllEntities.Count.ToString(), ": ");
                EntityCountText = LanguageManager.Combine("Entities", AllEntities.Count.ToString(), ": ");
            }
            catch (Exception ex)
            {
                EntityStatus = $"Scan Error: {ex.Message}";
            }
        }

        private EntityInfo? ReadEntityData(IntPtr entityAddr)
        {
            if (!_memory.IsReady || entityAddr == IntPtr.Zero) return null;
            var s = _memory.Settings;

            try
            {
                ushort entityId = _memory.ReadUInt16(entityAddr);
                byte entityType = _memory.ReadByte(entityAddr + s.EntityTypeOffset);
                byte subType = _memory.ReadByte(entityAddr + 0x03);

                if (!Enum.IsDefined(typeof(Settings.ENTITY_TYPE), entityType))
                    return null;

                int health = _memory.ReadInt32(entityAddr + s.EntityHealthOffset);
                Position pos = _memory.ReadEntityPosition(entityAddr);

                byte noCollide = _memory.ReadByte(entityAddr + 0x0D);
                byte invisible = _memory.ReadByte(entityAddr + 0x13);
                byte invincible = _memory.ReadByte(entityAddr + 0x27A);
                float mass = _memory.ReadFloat(entityAddr + 0x58);
                float friction = _memory.ReadFloat(entityAddr + 0x5C);
                byte glow = _memory.ReadByte(entityAddr + 0x70);
                int aiState = _memory.ReadInt32(entityAddr + 0x288);
                int aiWait = _memory.ReadInt32(entityAddr + 0x2A8);

                string typeName = GetTypeName(entityType);
                string subTypeName = GetSubTypeName(entityType, subType);

                return new EntityInfo
                {
                    BaseAddress = entityAddr,
                    EntityId = entityId,
                    EntityType = entityType,
                    TypeName = typeName,
                    SubType = subType,
                    SubTypeName = subTypeName,
                    Health = health,
                    Pos = pos,
                    NoCollide = noCollide,
                    Invisible = invisible,
                    Invincible = invincible,
                    Mass = mass,
                    Friction = friction,
                    Glow = glow,
                    AIState = aiState,
                    AIWait = aiWait,
                };
            }
            catch { return null; }
        }

        private string GetTypeName(byte entityType)
        {
            string typeName = "ENTITY_TYPE_UNKNOWN";
            if (Enum.IsDefined(typeof(Settings.ENTITY_TYPE), entityType))
            {
                var enumValue = (Settings.ENTITY_TYPE)Enum.ToObject(typeof(Settings.ENTITY_TYPE), entityType);
                typeName = enumValue.ToString();
            }
            return LanguageManager.Get(typeName);
        }

        private static string GetSubTypeName(byte entityType, byte subType)
        {
            if (entityType == (byte)Settings.ENTITY_TYPE.ENTITY_TYPE_ITEM)
            {
                return subType switch
                {
                    0x00 => LanguageManager.Get("Furniture"),
                    0x01 => LanguageManager.Get("Pickup"),
                    0x02 => LanguageManager.Get("Weapon"),
                    0x03 => LanguageManager.Get("Vehicle"),
                    0x04 => LanguageManager.Get("PickupSpec"),
                    _ => $"0x{subType:X2}"
                };
            }
            return "";
        }

        public void SelectEntity(EntityInfo? entity)
        {
            if (entity == null) return;
            CurrentEntity = entity;
            EntityStatus = $"Selected #{entity.EntityId} ({entity.TypeName})";
        }

        public void SetTarget()
        {
            if (_currentEntity == null)
            {
                TargetStatusText = LanguageManager.Get("NoTargetEntity");
                return;
            }
            TargetEntity = _currentEntity;
        }

        public void RefreshCurrentEntity()
        {
            if (_currentEntity == null || !_memory.IsReady) return;
            var refreshed = ReadEntityData(_currentEntity.BaseAddress);
            if (refreshed != null)
            {
                CurrentEntity = refreshed;
                EntityStatus = $"Refreshed #{refreshed.EntityId}";
            }
        }

        private void UpdateEntityUI(EntityInfo entity)
        {
            DetailEntityId = entity.EntityId;
            DetailTypeName = entity.TypeName;
            DetailSubTypeName = entity.SubTypeName;
            DetailHealth = entity.Health;
            DetailAreaId = entity.Pos.AreaId;
            DetailPosX = entity.Pos.PosX;
            DetailPosY = entity.Pos.PosY;
            DetailPosZ = entity.Pos.PosZ;
            DetailVelX = entity.Pos.VelX;
            DetailVelY = entity.Pos.VelY;
            DetailVelZ = entity.Pos.VelZ;
            DetailNoCollide = entity.NoCollide == 1;
            DetailInvisible = entity.Invisible == 1;
            DetailInvincible = entity.Invincible == 1;
            DetailMass = entity.Mass;
            DetailFriction = entity.Friction;
            DetailGlow = entity.Glow == 1;
            DetailAIState = entity.AIState;
            DetailAIWait = entity.AIWait;
        }

        public void ApplyEntityPosition()
        {
            if (_currentEntity == null || !_memory.IsReady) return;
            try
            {
                IntPtr addr = _currentEntity.BaseAddress;
                var s = _memory.Settings;

                _memory.WriteFloat(addr + s.EntityPosXOffset, DetailPosX);
                _memory.WriteFloat(addr + s.EntityPosYOffset, DetailPosY);
                _memory.WriteFloat(addr + s.EntityPosZOffset, DetailPosZ);
                _memory.WriteFloat(addr + s.EntityVelXOffset, DetailVelX);
                _memory.WriteFloat(addr + s.EntityVelYOffset, DetailVelY);
                _memory.WriteFloat(addr + s.EntityVelZOffset, DetailVelZ);

                _currentEntity.Pos.PosX = DetailPosX;
                _currentEntity.Pos.PosY = DetailPosY;
                _currentEntity.Pos.PosZ = DetailPosZ;
                _currentEntity.Pos.VelX = DetailVelX;
                _currentEntity.Pos.VelY = DetailVelY;
                _currentEntity.Pos.VelZ = DetailVelZ;

                EntityStatus = $"Position updated #{_currentEntity.EntityId}";
            }
            catch (Exception ex)
            {
                EntityStatus = $"Error: {ex.Message}";
            }
        }

        public void ApplyEntityHealth()
        {
            if (_currentEntity == null || !_memory.IsReady) return;
            _memory.WriteInt32(_currentEntity.BaseAddress + _memory.Settings.EntityHealthOffset, DetailHealth);
        }

        public void ApplyEntityDebug()
        {
            if (_currentEntity == null || !_memory.IsReady) return;
            try
            {
                IntPtr addr = _currentEntity.BaseAddress;
                _memory.WriteByte(addr + 0x0D, (byte)(DetailNoCollide ? 1 : 0));
                _memory.WriteByte(addr + 0x13, (byte)(DetailInvisible ? 1 : 0));
                _memory.WriteByte(addr + 0x27A, (byte)(DetailInvincible ? 1 : 0));
                _memory.WriteByte(addr + 0x70, (byte)(DetailGlow ? 1 : 0));
                _memory.WriteFloat(addr + 0x58, DetailMass);
                _memory.WriteFloat(addr + 0x5C, DetailFriction);
                _memory.WriteInt32(addr + 0x288, DetailAIState);
                _memory.WriteInt32(addr + 0x2A8, DetailAIWait);
                EntityStatus = $"Debug props applied to #{_currentEntity.EntityId}";
            }
            catch (Exception ex)
            {
                EntityStatus = $"Error: {ex.Message}";
            }
        }

        public void TeleportToTarget()
        {
            if (_currentEntity == null || _targetEntity == null) return;
            if (!_memory.IsReady) return;

            try
            {
                var pos = _memory.ReadEntityPosition(_targetEntity.BaseAddress);
                if (_memory.WriteEntityPosition(_currentEntity.BaseAddress, pos))
                {
                    EntityStatus = LanguageManager.Get("Teleported");
                    RefreshCurrentEntity();
                    ApplyFilter();
                }
            }
            catch (Exception ex)
            {
                EntityStatus = $"Error: {ex.Message}";
            }
        }

        public void SwapPositions()
        {
            if (_currentEntity == null || _targetEntity == null) return;
            if (!_memory.IsReady) return;

            try
            {
                IntPtr srcAddr = _currentEntity.BaseAddress;
                IntPtr dstAddr = _targetEntity.BaseAddress;
                var srcPos = _memory.ReadEntityPosition(srcAddr);
                var dstPos = _memory.ReadEntityPosition(dstAddr);
                if (_memory.WriteEntityPosition(srcAddr, dstPos) && _memory.WriteEntityPosition(dstAddr, srcPos))
                {
                    EntityStatus = LanguageManager.Get("Swapped");
                    RefreshCurrentEntity();
                    ApplyFilter();
                }
            }
            catch (Exception ex)
            {
                EntityStatus = $"Error: {ex.Message}";
            }
        }

        private void UpdateAreaFilter()
        {
            try
            {
                AreaOptions.Clear();
                AreaOptions.Add("All");
                if (AllEntities.Count > 0)
                {
                    var areas = AllEntities.Select(e => e.Pos.AreaId).Distinct().OrderBy(a => a);
                    foreach (var area in areas)
                        AreaOptions.Add(area.ToString());
                }
                AreaFilter = "All";
            }
            catch { }
        }

        private void ApplyFilter()
        {
            try
            {
                FilteredEntities.Clear();
                if (AllEntities.Count == 0) { EntityCountText = LanguageManager.Combine("Entities", "0", ": "); return; }

                var query = AllEntities.AsEnumerable();

                if (TypeFilterIndex > 0)
                {
                    byte filterType = TypeFilterIndex switch
                    {
                        1 => (byte)Settings.ENTITY_TYPE.ENTITY_TYPE_HUMAN,
                        2 => (byte)Settings.ENTITY_TYPE.ENTITY_TYPE_ZOMBIE,
                        3 => (byte)Settings.ENTITY_TYPE.ENTITY_TYPE_ITEM,
                        4 => (byte)Settings.ENTITY_TYPE.ENTITY_TYPE_PROJECTILE,
                        _ => 0
                    };
                    if (filterType > 0)
                        query = query.Where(e => e.EntityType == filterType);
                }

                if (AreaFilter != "All" && byte.TryParse(AreaFilter, out byte areaId))
                    query = query.Where(e => e.Pos.AreaId == areaId);

                if (!string.IsNullOrEmpty(SearchText))
                {
                    if (ushort.TryParse(SearchText, out ushort searchId))
                        query = query.Where(e => e.EntityId == searchId);
                    else
                        query = query.Where(e => e.TypeName.Contains(SearchText, StringComparison.OrdinalIgnoreCase));
                }

                foreach (var entity in query)
                    FilteredEntities.Add(entity);

                EntityCountText = $"{LanguageManager.Get("Entities")}: {FilteredEntities.Count} (from {AllEntities.Count})";
            }
            catch { }
        }

        private void UpdateTargetStatus()
        {
            if (_targetEntity != null)
                TargetStatusText = $"Target: #{_targetEntity.EntityId} ({_targetEntity.TypeName}) at ({_targetEntity.Pos.PosX:F2}, {_targetEntity.Pos.PosY:F2}, {_targetEntity.Pos.PosZ:F2})";
            else
                TargetStatusText = LanguageManager.Get("NoTargetEntity");
        }

        public bool BothEntitiesValid()
        {
            if (!_memory.IsReady) { return false; }
            if (_currentEntity == null) { EntityStatus = LanguageManager.Get("NoSourceEntity"); return false; }
            if (_targetEntity == null) { EntityStatus = LanguageManager.Get("NoTargetEntity"); return false; }
            if (_currentEntity.EntityId == _targetEntity.EntityId) { EntityStatus = LanguageManager.Get("SameSourceAndTarget"); return false; }
            return true;
        }
    }
}